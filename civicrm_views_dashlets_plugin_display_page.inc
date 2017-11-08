<?php

/**
 * @file
 * The plugin that handles dashlet displays.
 */

/**
 * Dashlet display plugin.
 */
class civicrm_views_dashlets_plugin_display_page extends views_plugin_display_page {

  /**
   * Define options and set default options.
   */
  public function option_definition() {
    $options = parent::option_definition();

    $options['path']['default'] = 'default';
    $options['cache_minutes'] = array('default' => '60');
    $options['id'] = array('default' => '');

    unset($options['menu']);

    return $options;
  }

  /**
   * This plugin does not use breadcrumbs.
   */
  public function uses_breadcrumb() {
    return FALSE;
  }

  /**
   * Provide the summary for page options in the views UI.
   *
   * This output is returned as an array.
   */
  public function options_summary(&$categories, &$options) {
    // It is very important to call the parent function here:
    parent::options_summary($categories, $options);

    $cache_minutes = $this->get_option('cache_minutes');

    $categories['page']['title'] = t('CiviCRM Dashlet settings');

    $options['title']['title'] = t('Dashlet title');
    if (!strlen($this->get_option('title'))) {
      $title = $this->_default_title();
      $options['title']['value'] = $title;
    }

    $options['cache_minutes'] = array(
      'category' => 'page',
      'title' => t('Dashlet caching (minutes)'),
      'value' => strlen($cache_minutes) ? $cache_minutes : 60,
    );

    unset($options['menu']);

    $path = $this->get_option('path');
    $path_option = $options['path'];
    if (!$path || $path == 'default') {
     $path_option['value'] = views_ui_truncate('/' . $this->_default_path(), 24);
    }
    $path_option['title'] = t('Ajax path');
    // Make this option appear last.
    unset($options['path']);
    $options['path'] = $path_option;
  }

  /**
   * Provide the default form for setting options.
   */
  public function options_form(&$form, &$form_state) {
    // It is very important to call the parent function here:
    parent::options_form($form, $form_state);

    switch ($form_state['section']) {
      case 'cache_minutes':
        $cache_minutes = $this->get_option('cache_minutes');
        $form['cache_minutes'] = array(
          '#type' => 'textfield',
          '#size' => 4,
          '#attributes' => array('placeholder' => 60, 'min' => 1),
          '#title' => t('Minutes to cache'),
          '#description' => t("How long before this view is automatically refreshed on the user's dashboard. This caching is done locally in each users' web browser. If you have a large number of users accessing this dashlet, consider also enabling caching of the View."),
          '#default_value' => $cache_minutes ? $cache_minutes : 60,
          '#element_validate' => array('element_validate_integer_positive'),
        );
        break;

      case 'path':
        $path = $this->get_option('path');
        if (!$path || $path == 'default') {
          $form['path']['#default_value'] = $this->_default_path();
        }
        $form['path']['#description'] = t('Internal ajax path used to load this dashlet. Rarely needs to be changed from the default.');
        break;

      case 'title':
        if (!strlen($this->get_option('title'))) {
          $form['title']['#default_value'] = $this->_default_title();
        }
        break;

      case 'access':
        if (isset($form['access']['type']['#options'])) {
          unset($form['access']['type']['#options']['role']);
          if ($form['access']['type']['#default_value'] == 'role') {
            $form['access']['type']['#default_value'] = 'none';
          }
        }
    }
  }

  /**
   * Set the option values to the values of form_state.
   */
  public function options_submit(&$form, &$form_state) {
    // It is very important to call the parent function here:
    parent::options_submit($form, $form_state);

    $section = $form_state['section'];

    switch ($section) {
      case 'cache_minutes':
        $this->set_option($section, $form_state['values'][$section]);
        break;
    }
  }

  /**
   * Validate.
   */
  public function validate() {
    $path = $this->get_option('path');
    if (!$path || $path == 'default') {
      $this->set_option('path', $this->_default_path());
    }

    $errors = parent::validate();

    $access = $this->get_option('access');
    if ($access['type'] == 'role') {
      $errors[] = t('CiviCRM dashlets do not support role-based access. Please choose a permission instead.');
    }
    
    $cache = $this->get_option('cache_minutes');
    if ($cache && !ctype_digit((string) $cache)) {
      $errors[] = t('Dashlet caching must be a positive integer.');
    }

    return $errors;
  }

  /**
   * Add this display's path information to Drupal's menu system.
   */
  public function execute_hook_menu($callbacks) {
    $items = array();
    // Replace % with the link to our standard views argument loader
    // views_arg_load -- which lives in views.module
    $bits = explode('/', $this->get_option('path'));
    $page_arguments = array($this->view->name, $this->display->id);
    $this->view->init_handlers();
    $view_arguments = $this->view->argument;

    // Replace % with %views_arg for menu autoloading and add to the
    // page arguments so the argument actually comes through.
    foreach ($bits as $pos => $bit) {
      if ($bit == '%') {
        $argument = array_shift($view_arguments);
        if (!empty($argument->options['specify_validation']) && $argument->options['validate']['type'] != 'none') {
          $bits[$pos] = '%views_arg';
        }
        $page_arguments[] = $pos;
      }
    }

    $path = implode('/', $bits);

    $access_plugin = $this->get_plugin('access');
    if (!isset($access_plugin)) {
      $access_plugin = views_get_plugin('access', 'none');
    }

    // Get access callback might return an array of the callback + the dynamic
    // arguments.
    $access_plugin_callback = $access_plugin->get_access_callback();

    if (is_array($access_plugin_callback)) {
      $access_arguments = array();

      // Find the plugin arguments.
      $access_plugin_method = array_shift($access_plugin_callback);
      $access_plugin_arguments = array_shift($access_plugin_callback);
      if (!is_array($access_plugin_arguments)) {
        $access_plugin_arguments = array();
      }

      $access_arguments[0] = array($access_plugin_method, &$access_plugin_arguments);

      // Move the plugin arguments to the access arguments array.
      $i = 1;
      foreach ($access_plugin_arguments as $key => $value) {
        if (is_int($value)) {
          $access_arguments[$i] = $value;
          $access_plugin_arguments[$key] = $i;
          $i++;
        }
      }
    }
    else {
      $access_arguments = array($access_plugin_callback);
    }

    if ($path) {
      $items[$path] = array(
        // Default views page entry.
        'page callback' => '_civicrm_views_dashlets_menu_callback',
        'page arguments' => $page_arguments,
        // Default access check (per display).
        'access callback' => 'views_access',
        'access arguments' => $access_arguments,
        // Identify URL embedded arguments and correlate them to a handler.
        'load arguments' => array(
          $this->view->name,
          $this->display->id,
          '%index',
        ),
        'delivery callback' => '_civicrm_views_dashlets_menu_delivery_callback',
      );
      $menu = $this->get_option('menu');
      if (empty($menu)) {
        $menu = array('type' => 'none');
      }
      // Set the title and description if we have one.
      if ($menu['type'] != 'none') {
        $items[$path]['title'] = $menu['title'];
        $items[$path]['description'] = $menu['description'];
      }

      if (isset($menu['weight'])) {
        $items[$path]['weight'] = intval($menu['weight']);
      }

      switch ($menu['type']) {
        case 'none':
        default:
          $items[$path]['type'] = MENU_CALLBACK;
          break;

        case 'normal':
          $items[$path]['type'] = MENU_NORMAL_ITEM;
          // Insert item into the proper menu.
          $items[$path]['menu_name'] = $menu['name'];
          break;

        case 'tab':
          $items[$path]['type'] = MENU_LOCAL_TASK;
          break;

        case 'default tab':
          $items[$path]['type'] = MENU_DEFAULT_LOCAL_TASK;
          break;
      }

      // Add context for contextual links.
      // @see menu_contextual_links()
      if (!empty($menu['context'])) {
        $items[$path]['context'] = !empty($menu['context_only_inline']) ? MENU_CONTEXT_INLINE : (MENU_CONTEXT_PAGE | MENU_CONTEXT_INLINE);
      }

      // If this is a 'default' tab, check to see if we have to create the
      // parent menu item.
      if ($menu['type'] == 'default tab') {
        $tab_options = $this->get_option('tab_options');
        if (!empty($tab_options['type']) && $tab_options['type'] != 'none') {
          $bits = explode('/', $path);
          // Remove the last piece.
          $bit = array_pop($bits);

          // We can't do this if they tried to make the last path bit variable.
          // @todo: We can validate this.
          if ($bit != '%views_arg' && !empty($bits)) {
            $default_path = implode('/', $bits);
            $items[$default_path] = array(
              // Default views page entry.
              'page callback' => '_civicrm_views_dashlets_menu_callback',
              'page arguments' => $page_arguments,
              // Default access check (per display).
              'access callback' => 'views_access',
              'access arguments' => $access_arguments,
              // Identify URL embedded arguments and correlate them to a
              // handler.
              'load arguments' => array(
                $this->view->name,
                $this->display->id,
                '%index',
              ),
              'title' => $tab_options['title'],
              'description' => $tab_options['description'],
              'menu_name' => $tab_options['name'],
              'delivery callback' => '_civicrm_views_dashlets_menu_delivery_callback',
            );
            switch ($tab_options['type']) {
              default:
              case 'normal':
                $items[$default_path]['type'] = MENU_NORMAL_ITEM;
                break;

              case 'tab':
                $items[$default_path]['type'] = MENU_LOCAL_TASK;
                break;
            }
            if (isset($tab_options['weight'])) {
              $items[$default_path]['weight'] = intval($tab_options['weight']);
            }
          }
        }
      }
    }

    return $items;
  }

  public function _default_path() {
    return $this->view->name . '_' . $this->display->id;
  }

  public function _default_title() {
    $title = $this->view->human_name;
    $number = 1;
    foreach ($this->view->display as $name => $display) {
      if ($name == $this->display->id) {
        break;
      }
      if ($display->display_plugin == 'dashlet_view') {
        ++$number;
      }
    }
    if ($number > 1) {
      $title .= " $number";
    }
    return $title;
  }

}
